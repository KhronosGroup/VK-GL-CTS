#ifndef _VKTDYNAMICSTATELINEWIDTHTESTS_HPP
#define _VKTDYNAMICSTATELINEWIDTHTESTS_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
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
 * \brief DYnamic State Line Width Tests.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

#include "vktTestCase.hpp"
#include "vkPipelineConstructionUtil.hpp"

namespace vkt
{
namespace DynamicState
{

class DynamicStateLWTests : public tcu::TestCaseGroup
{
public:
							DynamicStateLWTests		(tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType);
							DynamicStateLWTests		(const DynamicStateLWTests&) = delete;
							~DynamicStateLWTests	(void) = default;
	void					init					(void);
	DynamicStateLWTests&	operator=				(const DynamicStateLWTests&) = delete;

private:
	vk::PipelineConstructionType	m_pipelineConstructionType;
};

} // DynamicState
} // vkt

#endif // _VKTDYNAMICSTATELINEWIDTHTESTS_HPP
