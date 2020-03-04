#ifndef _GLCNEARESTEDGETESTS_HPP
#define _GLCNEARESTEDGETESTS_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2020 Valve Coporation.
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \file  glcNearestEdgeTests.hpp
 * \brief
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"

namespace glcts
{

class NearestEdgeCases : public deqp::TestCaseGroup
{
public:
	/* Public methods */
	NearestEdgeCases(deqp::Context& context);
	virtual ~NearestEdgeCases(void);

	void init(void);

private:
	/* Private methods */
	NearestEdgeCases(const NearestEdgeCases& other);
	NearestEdgeCases& operator=(const NearestEdgeCases& other);
};

} /* glcts namespace */

#endif // _GLCNEARESTEDGETESTS_HPP
