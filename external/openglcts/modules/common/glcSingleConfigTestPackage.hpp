#ifndef _GLCSINGLECONFIGTESTPACKAGE_HPP
#define _GLCSINGLECONFIGTESTPACKAGE_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2016 Google Inc.
 * Copyright (c) 2016-2019 The Khronos Group Inc.
 * Copyright (c) 2019 NVIDIA Corporation.
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
 * \brief OpenGL/OpenGL ES Test Package that only gets run in a single config
 */ /*-------------------------------------------------------------------*/

#include "glcTestPackage.hpp"
#include "tcuDefs.hpp"

namespace glcts
{

class SingleConfigTestPackage : public deqp::TestPackage
{
public:
	SingleConfigTestPackage(tcu::TestContext& testCtx, const char* packageName,
							glu::ContextType renderContextType);
	~SingleConfigTestPackage(void);

	void init(void);

	virtual tcu::TestCaseExecutor* createExecutor(void) const;

private:
	SingleConfigTestPackage(const SingleConfigTestPackage& other);
	SingleConfigTestPackage& operator=(const SingleConfigTestPackage& other);
};

} // glcts

#endif // _GLCSINGLECONFIGTESTPACKAGE_HPP
