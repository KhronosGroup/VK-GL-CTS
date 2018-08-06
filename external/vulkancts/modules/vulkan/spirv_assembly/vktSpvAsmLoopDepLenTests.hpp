#ifndef _VKTSPVASMLOOPDEPLENTESTS_HPP
#define _VKTSPVASMLOOPDEPLENTESTS_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
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
 * \brief SPIR-V Loop Control for DependencyLength qualifier tests
 *//*--------------------------------------------------------------------*/

#include "vkPrograms.hpp"
#include "vktTestCase.hpp"

#include "vktSpvAsmComputeShaderTestUtil.hpp"

namespace vkt
{
namespace SpirVAssembly
{

class SpvAsmLoopControlDependencyLengthCase : public TestCase
{
public:
							SpvAsmLoopControlDependencyLengthCase	(tcu::TestContext& testCtx, const char* name, const char* description);
	void					initPrograms							(vk::SourceCollections& programCollection) const;
	TestInstance*			createInstance							(Context& context) const;
};


} // SpirVAssembly
} // vkt

#endif // _VKTSPVASMLOOPDEPLENTESTS_HPP
