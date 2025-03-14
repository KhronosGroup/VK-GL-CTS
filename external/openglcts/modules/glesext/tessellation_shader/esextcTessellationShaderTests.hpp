#ifndef _ESEXTCTESSELLATIONSHADERTESTS_HPP
#define _ESEXTCTESSELLATIONSHADERTESTS_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2014-2016 The Khronos Group Inc.
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

#include "../esextcTestCaseBase.hpp"
#include "gluShaderUtil.hpp"
#include "tcuDefs.hpp"

namespace glcts
{

class TessellationShaderTests : public glcts::TestCaseGroupBase
{
public:
    /* Public methods */
    TessellationShaderTests(glcts::Context &context, const ExtParameters &extParams);

    virtual ~TessellationShaderTests(void)
    {
    }

    void init(void);

private:
    /* Private methods */
    TessellationShaderTests(const TessellationShaderTests &other);
    TessellationShaderTests &operator=(const TessellationShaderTests &other);

    void addTessellationShaderVertexSpacingTest(TestCaseGroupBase *vertexGroup);
};

} // namespace glcts

#endif // _ESEXTCTESSELLATIONSHADERTESTS_HPP
