#ifndef _GLCPIXELSTORAGEMODESTESTS_HPP
#define _GLCPIXELSTORAGEMODESTESTS_HPP

/*-------------------------------------------------------------------------
* OpenGL Conformance Test Suite
* -----------------------------
*
* Copyright (c) 2020 The Khronos Group Inc.
* Copyright (c) 2020 Intel Corporation
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
* \file  glcPixelStorageModesTests.hpp
* \brief Conformance tests for usage of pixel storage modes
*/ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"
#include "gluShaderUtil.hpp"
#include "glwDefs.hpp"
#include "tcuDefs.hpp"

namespace glcts
{
	class PixelStorageModesTests : public deqp::TestCaseGroup
{
public:
	PixelStorageModesTests	(deqp::Context& context, glu::GLSLVersion glsl_version);
	~PixelStorageModesTests	(void);

	glu::GLSLVersion m_glsl_version;

	void init (void);
private:

	PixelStorageModesTests (const PixelStorageModesTests& other);
	PixelStorageModesTests& operator= (const PixelStorageModesTests& other);
};

} // glcts

#endif // _GLCPIXELSTORAGEMODESTESTS_HPP
