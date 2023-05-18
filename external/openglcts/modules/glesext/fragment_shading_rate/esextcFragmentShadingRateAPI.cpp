/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2022-2022 The Khronos Group Inc.
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
 */

/*!
 * \file  esextFragmentShadingRateAPI.cpp
 * \brief Base test group for fragment shading rate tests
 */ /*-------------------------------------------------------------------*/

#include "esextcFragmentShadingRateAPI.hpp"
#include "esextcFragmentShadingRateBasic.hpp"
#include "esextcFragmentShadingRateErrors.hpp"

namespace glcts
{

/// Constructor
///
/// @param context       Test context
/// @param glslVersion   GLSL version
FragmentShadingRateAPI::FragmentShadingRateAPI(glcts::Context& context, const ExtParameters& extParams)
	: TestCaseGroupBase(context, extParams, "api", "Fragment Shading Rate")
{
}

/// Initializes test cases for fragment shading rate tests
void FragmentShadingRateAPI::init(void)
{
	// Initialize base class
	TestCaseGroupBase::init();

	// Case 1 - via basic shading rate function
	addChild(new FragmentShadingRateBasic(m_context, m_extParams, "basic", "Fragment shading rate functional test"));
	// Case 2 - validation error check
	addChild(new FragmentShadingRateErrors(m_context, m_extParams, "errors",
										   "Fragment Shading Rate Validation Error Tests"));
}

} // namespace glcts
