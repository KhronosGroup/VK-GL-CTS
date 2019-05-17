#ifndef _VKTSPVASMFLOATCONTROLSEXTENSIONLESSTESTS_HPP
#define _VKTSPVASMFLOATCONTROLSEXTENSIONLESSTESTS_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief SPIR-V Float Control SPIR-V tokens test
 *//*--------------------------------------------------------------------*/

#include "vkPrograms.hpp"
#include "vktTestCase.hpp"

#include "vktSpvAsmComputeShaderTestUtil.hpp"

namespace vkt
{
namespace SpirVAssembly
{

class SpvAsmFloatControlsExtensionlessCase : public TestCase
{
public:
					SpvAsmFloatControlsExtensionlessCase	(tcu::TestContext& testCtx, const char* name, const char* description, const char* featureName, const int fpWideness, const bool spirv14);
	void			initPrograms							(vk::SourceCollections& programCollection) const;
	TestInstance*	createInstance							(Context& context) const;
	virtual void	checkSupport							(Context& context) const;

protected:
	const char*		m_featureName;
	const int		m_fpWideness;
	const bool		m_spirv14;
};

tcu::TestCaseGroup* createFloatControlsExtensionlessGroup (tcu::TestContext& testCtx);

} // SpirVAssembly
} // vkt

#endif // _VKTSPVASMFLOATCONTROLSEXTENSIONLESSTESTS_HPP
