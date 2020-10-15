#ifndef _VKTSPVASMCOMPUTESHADERCASE_HPP
#define _VKTSPVASMCOMPUTESHADERCASE_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Test Case Skeleton Based on Compute Shaders
 *//*--------------------------------------------------------------------*/

#include "vkPrograms.hpp"
#include "vktTestCase.hpp"

#include "vktSpvAsmComputeShaderTestUtil.hpp"

namespace vkt
{
namespace SpirVAssembly
{
/*--------------------------------------------------------------------*//*!
 * \brief Test instance for compute pipeline
 *
 * The compute shader is specified in the format of SPIR-V assembly, which
 * is allowed to access MAX_NUM_INPUT_BUFFERS input storage buffers and
 * MAX_NUM_OUTPUT_BUFFERS output storage buffers maximally. The shader
 * source and input/output data are given in a ComputeShaderSpec object.
 *
 * This instance runs the given compute shader by feeding the data from input
 * buffers and compares the data in the output buffers with the expected.
 *//*--------------------------------------------------------------------*/
class SpvAsmComputeShaderInstance : public TestInstance
{
public:
										SpvAsmComputeShaderInstance	(Context& ctx, const ComputeShaderSpec& spec);
	tcu::TestStatus						iterate						(void);

private:
	const ComputeShaderSpec&			m_shaderSpec;
};

class SpvAsmComputeShaderCase : public TestCase
{
public:
						SpvAsmComputeShaderCase	(tcu::TestContext& testCtx, const char* name, const char* description, const ComputeShaderSpec& spec);
	void				checkSupport			(Context& context) const;
	void				initPrograms			(vk::SourceCollections& programCollection) const;
	TestInstance*		createInstance			(Context& ctx) const;

private:
	ComputeShaderSpec	m_shaderSpec;
};

} // SpirVAssembly
} // vkt

#endif // _VKTSPVASMCOMPUTESHADERCASE_HPP
