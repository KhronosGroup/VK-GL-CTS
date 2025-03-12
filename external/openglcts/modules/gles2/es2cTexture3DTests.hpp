#ifndef _ES2CTEXTURE3DTESTS_HPP
#define _ES2CTEXTURE3DTESTS_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
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
 */ /*!
 * \file es2cTexture3DTests.hpp
 * \brief GL_OES_texture_3D tests declaration.
 */ /*-------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tes31TestCase.hpp"

namespace es2cts
{

class Texture3DTests : public deqp::TestCaseGroup
{
public:
    Texture3DTests(deqp::Context &context);
    ~Texture3DTests(void);
    void init(void);

private:
    Texture3DTests(const Texture3DTests &other);
    Texture3DTests &operator=(const Texture3DTests &other);
};
} // namespace es2cts

#endif // _ES2CTEXTURE3DTESTS_HPP
