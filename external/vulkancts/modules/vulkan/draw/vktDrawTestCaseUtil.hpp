#ifndef _VKTDRAWTESTCASEUTIL_HPP
#define _VKTDRAWTESTCASEUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
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