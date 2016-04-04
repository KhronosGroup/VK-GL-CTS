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
#include "tcuResource.hpp"

#include "vktTestCase.hpp"

#include "gluShaderUtil.hpp"
#include "vkPrograms.hpp"

#include <map>

namespace vkt
{
namespace Draw
{

class ShaderSourceProvider
{
public:
	static std::string getSource (tcu::Archive& archive, const char* path)
	{
		tcu::Resource *resource = archive.getResource(path);

		std::vector<deUint8> readBuffer(resource->getSize() + 1);
		resource->read(&readBuffer[0], resource->getSize());
		readBuffer[readBuffer.size() - 1] = 0;

		return reinterpret_cast<const char*>(&readBuffer[0]);
	}
};

typedef std::map<glu::ShaderType, const char*> ShaderMap;

template<typename Instance>
class InstanceFactory : public TestCase
{
public:
	InstanceFactory (tcu::TestContext& testCtx, const std::string& name, const std::string& desc,
		const std::map<glu::ShaderType, const char*> shaderPaths, const vk::VkPrimitiveTopology topology)
		: TestCase		(testCtx, name, desc)
		, m_shaderPaths (shaderPaths)
		, m_topology	(topology)
	{
	}

	TestInstance* createInstance (Context& context) const
	{
		return new Instance(context, m_shaderPaths, m_topology);
	}

	virtual void initPrograms (vk::SourceCollections& programCollection) const
	{
		for (ShaderMap::const_iterator i = m_shaderPaths.begin(); i != m_shaderPaths.end(); ++i)
		{
			programCollection.glslSources.add(i->second) <<
				glu::ShaderSource(i->first, ShaderSourceProvider::getSource(m_testCtx.getArchive(), i->second));
		}
	}

private:
	const ShaderMap m_shaderPaths;
	const vk::VkPrimitiveTopology m_topology;
};

} // Draw
} // vkt

#endif // _VKTDRAWTESTCASEUTIL_HPP