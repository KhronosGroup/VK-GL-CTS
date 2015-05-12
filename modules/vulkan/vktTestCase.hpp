#ifndef _VKTTESTCASE_HPP
#define _VKTTESTCASE_HPP
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
 * \brief Vulkan test case base classes
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

namespace glu
{
struct ProgramSources;
}

namespace vk
{
class PlatformInterface;
class ProgramBinary;
template<typename Program> class ProgramCollection;
}

namespace vkt
{

class Context
{
public:
												Context				(tcu::TestContext&							testCtx,
																	 const vk::PlatformInterface&				platformInterface,
																	 vk::ProgramCollection<vk::ProgramBinary>&	progCollection)
																		: m_testCtx				(testCtx)
																		, m_platformInterface	(platformInterface)
																		, m_progCollection		(progCollection) {}
												~Context			(void) {}

	tcu::TestContext&							getTestContext		(void) const { return m_testCtx;			}
	const vk::PlatformInterface&				getPlatformInterface(void) const { return m_platformInterface;	}
	vk::ProgramCollection<vk::ProgramBinary>&	getBinaryCollection	(void) const { return m_progCollection;		}

protected:
	tcu::TestContext&							m_testCtx;
	const vk::PlatformInterface&				m_platformInterface;
	vk::ProgramCollection<vk::ProgramBinary>&	m_progCollection;
};

class TestInstance;

class TestCase : public tcu::TestCase
{
public:
							TestCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description);
							TestCase		(tcu::TestContext& testCtx, tcu::TestNodeType type, const std::string& name, const std::string& description);
	virtual					~TestCase		(void) {}

	virtual void			initPrograms	(vk::ProgramCollection<glu::ProgramSources>& programCollection) const;
	virtual TestInstance*	createInstance	(Context& context) const = 0;

	IterateResult			iterate			(void) { DE_ASSERT(false); return STOP; } // Deprecated in this module
};

class TestInstance
{
public:
								TestInstance	(Context& context) : m_context(context) {}
	virtual						~TestInstance	(void) {}

	virtual tcu::TestStatus		iterate			(void) = 0;

protected:
	Context&					m_context;
};

inline TestCase::TestCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description)
	: tcu::TestCase(testCtx, name.c_str(), description.c_str())
{
}

inline TestCase::TestCase (tcu::TestContext& testCtx, tcu::TestNodeType type, const std::string& name, const std::string& description)
	: tcu::TestCase(testCtx, type, name.c_str(), description.c_str())
{
}

} // vkt

#endif // _VKTTESTCASE_HPP
