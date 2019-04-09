#ifndef _ESEXTCTEXTURESHADOWLODFUNCTIONSTEST_HPP
#define _ESEXTCTEXTURESHADOWLODFUNCTIONSTEST_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2014-2019 The Khronos Group Inc.
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
 */ /*!
 * \file
 * \brief
 */ /*-------------------------------------------------------------------*/

/*!
 * \file esextcTextureShadowLodFunctionsTest.hpp
 * \brief EXT_texture_shadow_lod extension testing
 */ /*-------------------------------------------------------------------*/

#include "tes3TestCase.hpp"

namespace deqp
{
namespace Functional
{

class TextureShadowLodTest : public TestCaseGroup
{
public:
	TextureShadowLodTest(Context& context);
	virtual ~TextureShadowLodTest(void);

	virtual void init(void);

private:
	TextureShadowLodTest(const TextureShadowLodTest&);			  // not allowed!
	TextureShadowLodTest& operator=(const TextureShadowLodTest&); // not allowed!
};

} // namespace Functional
} // namespace deqp

#endif // _ESEXTCTEXTURESHADOWLODFUNCTIONSTEST_HPP
