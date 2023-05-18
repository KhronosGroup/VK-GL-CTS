#ifndef _VKTDRAWTESTCASEUTIL_HPP
#define _VKTDRAWTESTCASEUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
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
 * \brief Draw Test Case Utils
 *//*--------------------------------------------------------------------*/


#include "tcuDefs.hpp"

#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktDrawGroupParams.hpp"

#include "gluShaderUtil.hpp"
#include "vkPrograms.hpp"

#include "deUniquePtr.hpp"

#include <map>

namespace vkt
{
namespace Draw
{

typedef std::map<glu::ShaderType, const char*> ShaderMap;

struct TestSpecBase
{
	ShaderMap					shaders;
	vk::VkPrimitiveTopology		topology;
	const SharedGroupParams		groupParams;
};

template<typename Instance, typename Support = NoSupport0>
class InstanceFactory : public TestCase
{
public:
	InstanceFactory (tcu::TestContext& testCtx, const std::string& name, const std::string& desc, typename Instance::TestSpec testSpec)
		: TestCase		(testCtx, name, desc)
		, m_testSpec	(testSpec)
		, m_support		()
	{
	}

	InstanceFactory (tcu::TestContext& testCtx, const std::string& name, const std::string& desc, typename Instance::TestSpec testSpec, const Support& support)
		: TestCase		(testCtx, name, desc)
		, m_testSpec	(testSpec)
		, m_support		(support)
	{
	}

	TestInstance*	createInstance	(Context& context) const
	{
		return new Instance(context, m_testSpec);
	}

	virtual void	initPrograms	(vk::SourceCollections& programCollection) const
	{
		for (ShaderMap::const_iterator i = m_testSpec.shaders.begin(); i != m_testSpec.shaders.end(); ++i)
		{
			programCollection.glslSources.add(i->second) <<
				glu::ShaderSource(i->first, ShaderSourceProvider::getSource(m_testCtx.getArchive(), i->second));
		}
	}

	virtual void	checkSupport	(Context& context) const
	{
		m_support.checkSupport(context);
	}

private:
	const typename Instance::TestSpec	m_testSpec;
	const Support						m_support;
};

} // Draw
} // vkt

#endif // _VKTDRAWTESTCASEUTIL_HPP
