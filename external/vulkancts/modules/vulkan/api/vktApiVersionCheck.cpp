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
* \brief API Version Check test - prints out version info
*//*--------------------------------------------------------------------*/

#include <iostream>
#include <typeinfo>

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"
#include "tcuFunctionLibrary.hpp"
#include "tcuPlatform.hpp"

#include "vkApiVersion.hpp"
#include "vkDefs.hpp"
#include "vkPlatform.hpp"

#include "vktApiVersionCheck.hpp"
#include "vktTestCase.hpp"

#include "deString.h"
#include "deStringUtil.hpp"

#include <map>
#include <vector>

using namespace vk;
using namespace std;

namespace vkt
{

namespace api
{

namespace
{

#include "vkCoreFunctionalities.inl"

class APIVersionTestInstance : public TestInstance
{
public:
								APIVersionTestInstance	(Context&				ctx)
									: TestInstance	(ctx)
	{}
	virtual tcu::TestStatus		iterate					(void)
	{
		tcu::TestLog&			log				= m_context.getTestContext().getLog();
		const vk::ApiVersion	instanceVersion	= vk::unpackVersion(m_context.getInstanceVersion());
		const vk::ApiVersion	deviceVersion	= vk::unpackVersion(m_context.getDeviceVersion());
		const vk::ApiVersion	usedApiVersion	= vk::unpackVersion(m_context.getUsedApiVersion());

		log << tcu::TestLog::Message << "instanceVersion: " << instanceVersion << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "deviceVersion: " << deviceVersion << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "usedApiVersion: " << usedApiVersion << tcu::TestLog::EndMessage;
		const ::std::string		result			= de::toString(usedApiVersion.majorNum) + ::std::string(".") + de::toString(usedApiVersion.minorNum) + ::std::string(".") + de::toString(usedApiVersion.patchNum);
		return tcu::TestStatus::pass(result);
	}
};

class APIVersionTestCase : public TestCase
{
public:
							APIVersionTestCase	(tcu::TestContext&		testCtx)
								: TestCase	(testCtx, "version", "Prints out API info.")
	{}

	virtual					~APIVersionTestCase	(void)
	{}
	virtual TestInstance*	createInstance		(Context&				ctx) const
	{
		return new APIVersionTestInstance(ctx);
	}

private:
};

class APIEntryPointsTestInstance : public TestInstance
{
public:
								APIEntryPointsTestInstance	(Context&				ctx)
									: TestInstance	(ctx)
	{

	}
	virtual tcu::TestStatus		iterate						(void)
	{
		tcu::TestLog&						log				= m_context.getTestContext().getLog();
		const vk::Platform&					platform		= m_context.getTestContext().getPlatform().getVulkanPlatform();
		de::MovePtr<vk::Library>			vkLibrary		= de::MovePtr<vk::Library>(platform.createLibrary());
		const tcu::FunctionLibrary&			funcLibrary		= vkLibrary->getFunctionLibrary();
		getInstanceProcAddr									= (GetInstanceProcAddrFunc)funcLibrary.getFunction("vkGetInstanceProcAddr");
		getDeviceProcAddr									= (GetDeviceProcAddrFunc)getInstanceProcAddr(m_context.getInstance(), "vkGetDeviceProcAddr");

		deUint32							failsQuantity	= 0u;
		ApisMap								functions		= ApisMap();
		initApisMap(functions);

		ApisMap::const_iterator				lastGoodVersion	= functions.begin();
		for (ApisMap::const_iterator it = lastGoodVersion; it != functions.end(); ++it)
		{
			if (it->first <= m_context.getUsedApiVersion())
				lastGoodVersion	= it;
		}
		failChecking(log, failsQuantity, lastGoodVersion->second);

		if (failsQuantity > 0u)
			return tcu::TestStatus::fail("Fail");
		else
			return tcu::TestStatus::pass("Pass");
	}

private:

	GetDeviceProcAddrFunc	getDeviceProcAddr;
	GetInstanceProcAddrFunc getInstanceProcAddr;

	void failChecking (tcu::TestLog& log, deUint32& failsQuantity, const vector<pair<const char*, FunctionOrigin> >& testsArr)
	{
		for (deUint32 ndx = 0u; ndx < testsArr.size(); ++ndx)
		{
			if (!deStringEqual(testsArr[ndx].first, "vkGetInstanceProcAddr") &&
				!deStringEqual(testsArr[ndx].first, "vkEnumerateInstanceVersion"))
			{
				const deUint32 functionType	= testsArr[ndx].second;
				switch (functionType)
				{

					case FUNCTIONORIGIN_PLATFORM:
						if (getInstanceProcAddr(DE_NULL, testsArr[ndx].first) == DE_NULL)
						{
							log << tcu::TestLog::Message << \
								"[" << failsQuantity << "]\tptr to function '" << testsArr[ndx].first \
								<< "'  == DE_NULL" << \
								tcu::TestLog::EndMessage;
							failsQuantity++;
						}
						break;

					case FUNCTIONORIGIN_INSTANCE:
						if (getInstanceProcAddr(m_context.getInstance(), testsArr[ndx].first) == DE_NULL)
						{
							log << tcu::TestLog::Message << \
								"[" << failsQuantity << "]\tptr to function '" << testsArr[ndx].first \
								<< "'  == DE_NULL" << \
								tcu::TestLog::EndMessage;
							failsQuantity++;
						}
						break;

					case FUNCTIONORIGIN_DEVICE:
						if (getDeviceProcAddr(m_context.getDevice(), testsArr[ndx].first) == DE_NULL)
						{
							log << tcu::TestLog::Message << \
								"[" << failsQuantity << "]\tptr to function '" << testsArr[ndx].first \
								<< "'  == DE_NULL" << \
								tcu::TestLog::EndMessage;
							failsQuantity++;
						}
						break;

					default:
						DE_FATAL("Unrecognised function origin. Functions must come from Platform, Instance or Driver.");
				}
			}
		}
	}
};

class APIEntryPointsTestCase : public TestCase
{
public:
							APIEntryPointsTestCase			(tcu::TestContext&		testCtx)
								: TestCase	(testCtx, "entry_points", "Prints out API info.")
	{}

	virtual					~APIEntryPointsTestCase			(void)
	{}
	virtual TestInstance*	createInstance					(Context&				ctx) const
	{
		return new APIEntryPointsTestInstance(ctx);
	}

private:
};

} // anonymous

tcu::TestCaseGroup*			createVersionSanityCheckTests	(tcu::TestContext & testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	versionTests	(new tcu::TestCaseGroup(testCtx, "version_check", "API Version Tests"));
	versionTests->addChild(new APIVersionTestCase(testCtx));
	versionTests->addChild(new APIEntryPointsTestCase(testCtx));
	return versionTests.release();
}

} // api

} // vkt
