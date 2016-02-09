#ifndef _VKTTESTCASE_HPP
#define _VKTTESTCASE_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
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
 * \brief Vulkan test case base classes
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "vkDefs.hpp"
#include "deUniquePtr.hpp"

namespace glu
{
struct ProgramSources;
}

namespace vk
{
class PlatformInterface;
class ProgramBinary;
template<typename Program> class ProgramCollection;
class Allocator;
struct SourceCollections;
}

namespace vkt
{

class DefaultDevice;

class Context
{
public:
												Context							(tcu::TestContext&							testCtx,
																				 const vk::PlatformInterface&				platformInterface,
																				 vk::ProgramCollection<vk::ProgramBinary>&	progCollection);
												~Context						(void);

	tcu::TestContext&							getTestContext					(void) const { return m_testCtx;			}
	const vk::PlatformInterface&				getPlatformInterface			(void) const { return m_platformInterface;	}
	vk::ProgramCollection<vk::ProgramBinary>&	getBinaryCollection				(void) const { return m_progCollection;		}

	// Default instance & device, selected with --deqp-vk-device-id=N
	vk::VkInstance								getInstance						(void) const;
	const vk::InstanceInterface&				getInstanceInterface			(void) const;
	vk::VkPhysicalDevice						getPhysicalDevice				(void) const;
	const vk::VkPhysicalDeviceFeatures&			getDeviceFeatures				(void) const;
	const vk::VkPhysicalDeviceProperties&		getDeviceProperties				(void) const;
	vk::VkDevice								getDevice						(void) const;
	const vk::DeviceInterface&					getDeviceInterface				(void) const;
	deUint32									getUniversalQueueFamilyIndex	(void) const;
	vk::VkQueue									getUniversalQueue				(void) const;

	vk::Allocator&								getDefaultAllocator				(void) const;

protected:
	tcu::TestContext&							m_testCtx;
	const vk::PlatformInterface&				m_platformInterface;
	vk::ProgramCollection<vk::ProgramBinary>&	m_progCollection;

	const de::UniquePtr<DefaultDevice>			m_device;
	const de::UniquePtr<vk::Allocator>			m_allocator;

private:
												Context							(const Context&); // Not allowed
	Context&									operator=						(const Context&); // Not allowed
};


class TestInstance;

class TestCase : public tcu::TestCase
{
public:
							TestCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description);
							TestCase		(tcu::TestContext& testCtx, tcu::TestNodeType type, const std::string& name, const std::string& description);
	virtual					~TestCase		(void) {}

	virtual void			initPrograms	(vk::SourceCollections& programCollection) const;
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

private:
								TestInstance	(const TestInstance&);
	TestInstance&				operator=		(const TestInstance&);
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
