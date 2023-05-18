#ifndef _VKTDYNAMICSTATEDISCARDTESTS_HPP
#define _VKTDYNAMICSTATEDISCARDTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 LunarG, Inc.
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 Google LLC
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
 * \brief Dynamic State Discard Tests
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "vktTestCase.hpp"
#include "vkPipelineConstructionUtil.hpp"

namespace vkt
{
namespace DynamicState
{

class DynamicStateDiscardTests : public tcu::TestCaseGroup
{
public:
					DynamicStateDiscardTests	(tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType);
					~DynamicStateDiscardTests	(void);
	void			init						(void);

private:
	DynamicStateDiscardTests					(const DynamicStateDiscardTests& other);
	DynamicStateDiscardTests&		operator=	(const DynamicStateDiscardTests& other);

	vk::PipelineConstructionType	m_pipelineConstructionType;
};

} // DynamicState
} // vkt

#endif // _VKTDYNAMICSTATEDISCARDTESTS_HPP
