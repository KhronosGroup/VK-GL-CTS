#ifndef _VKTRAYQUERYBARYCENTRICCOORDINATESTESTS_HPP
#define _VKTRAYQUERYBARYCENTRICCOORDINATESTESTS_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Valve Corporation.
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
 * \brief Ray Query Barycentric Coordinates Tests
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

namespace vkt
{
namespace RayQuery
{

tcu::TestCaseGroup*	createBarycentricCoordinatesTests (tcu::TestContext& testCtx);

} // RayQuery
} // vkt

#endif // _VKTRAYQUERYBARYCENTRICCOORDINATESTESTS_HPP
