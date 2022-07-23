#ifndef _ESEXTCFRAGMENTSHADINGRATETESTS_HPP
#define _ESEXTCFRAGMENTSHADINGRATETESTS_HPP
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
 *
 */

/*!
 * \file  esextFragmentShadingRateTests.hpp
 * \brief Base test group for fragment shading rate tests
 */ /*-------------------------------------------------------------------*/

#include "../esextcTestCaseBase.hpp"
#include "gluShaderUtil.hpp"
#include "tcuDefs.hpp"

namespace glcts
{

namespace fsrutils
{

deUint32 packShadingRate(glw::GLenum shadingRate);

} // namespace fsrutils

// Base test group for fragment shading tests
class FragmentShadingRateTests : public TestCaseGroupBase
{
public:
	FragmentShadingRateTests(glcts::Context& context, const ExtParameters& extParams);

	~FragmentShadingRateTests(void) override
	{
	}

	void init(void) override;

private:
	FragmentShadingRateTests(const FragmentShadingRateTests& other);
	FragmentShadingRateTests& operator=(const FragmentShadingRateTests& other);
};

} // namespace glcts

#endif // _ESEXTCFRAGMENTSHADINGRATETESTS_HPP
