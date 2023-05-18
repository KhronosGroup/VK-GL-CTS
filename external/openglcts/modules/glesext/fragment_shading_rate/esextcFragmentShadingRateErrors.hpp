#ifndef _ESEXTCFRAGMENTSHADINGRATEERRORS_HPP
#define _ESEXTCFRAGMENTSHADINGRATEERRORS_HPP
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
 * \file  esextcesextcFragmentShadingRateErrors.hpp
 * \brief FragmentShadingRateEXT errors
 */ /*-------------------------------------------------------------------*/

#include "../esextcTestCaseBase.hpp"
#include "gluShaderUtil.hpp"
#include "tcuDefs.hpp"

#include <vector>

namespace glcts
{
class FragmentShadingRateErrors : public TestCaseBase
{
public:
	FragmentShadingRateErrors(Context& context, const ExtParameters& extParams, const char* name,
							  const char* description);

	~FragmentShadingRateErrors() override
	{
	}
	void				  init(void) override;
	void				  deinit(void) override;
	IterateResult		  iterate(void) override;

private:
	glw::GLboolean verifyError(const glw::GLenum expected_error, const char* description) const;
};

} // namespace glcts

#endif // _ESEXTCFRAGMENTSHADINGRATEERRORS_HPP
