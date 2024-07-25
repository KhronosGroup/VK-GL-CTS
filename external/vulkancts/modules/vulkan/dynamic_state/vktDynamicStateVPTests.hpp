#ifndef _VKTDYNAMICSTATEVPTESTS_HPP
#define _VKTDYNAMICSTATEVPTESTS_HPP
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
 * \brief Dynamic State Viewport Tests
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "vktTestCase.hpp"
#include "vkPipelineConstructionUtil.hpp"

namespace vkt
{
namespace DynamicState
{

class DynamicStateVPTests : public tcu::TestCaseGroup
{
public:
    DynamicStateVPTests(tcu::TestContext &testCtx, vk::PipelineConstructionType pipelineConstructionType);
    ~DynamicStateVPTests(void);
    void init(void);

private:
    DynamicStateVPTests(const DynamicStateVPTests &other);
    DynamicStateVPTests &operator=(const DynamicStateVPTests &other);

    vk::PipelineConstructionType m_pipelineConstructionType;
};

} // namespace DynamicState
} // namespace vkt

#endif // _VKTDYNAMICSTATEVPTESTS_HPP
