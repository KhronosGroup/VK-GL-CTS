#ifndef _TEGLAPICASE_HPP
#define _TEGLAPICASE_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program EGL Module
 * ---------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
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
 * \brief API test case.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "teglTestCase.hpp"
#include "egluCallLogWrapper.hpp"
#include "tcuTestLog.hpp"
#include "egluConfigFilter.hpp"

namespace deqp
{
namespace egl
{

class ApiCase : public TestCase, protected eglu::CallLogWrapper
{
public:
						ApiCase					(EglTestContext& eglTestCtx, const char* name, const char* description);
	virtual				~ApiCase				(void);

	IterateResult		iterate					(void);

protected:
	virtual void		test					(void) = DE_NULL;

	void				expectError				(EGLenum error);
	void				expectBoolean			(EGLBoolean expected, EGLBoolean got);

	void				expectNoContext			(EGLContext got);
	void				expectNoSurface			(EGLSurface got);
	void				expectNoDisplay			(EGLDisplay got);
	void				expectNull				(const void* got);

	inline void			expectTrue				(EGLBoolean got) { expectBoolean(EGL_TRUE, got); }
	inline void			expectFalse				(EGLBoolean got) { expectBoolean(EGL_FALSE, got); }

	bool				isAPISupported			(EGLenum api) const	{ return m_eglTestCtx.isAPISupported(api);			}
	EGLDisplay			getDisplay				(void)				{ return m_eglTestCtx.getDisplay().getEGLDisplay(); }
	bool				getConfig				(EGLConfig* cfg, const eglu::FilterList& filters);
};

} // egl
} // deqp

// Helper macro for declaring ApiCases.
#define TEGL_ADD_API_CASE(NAME, DESCRIPTION, TEST_FUNC_BODY)									\
	do {																						\
		class ApiCase_##NAME : public deqp::egl::ApiCase {										\
		public:																					\
			ApiCase_##NAME (EglTestContext& context) : ApiCase(context, #NAME, DESCRIPTION) {}	\
		protected:																				\
			void test (void) TEST_FUNC_BODY														\
		};																						\
		addChild(new ApiCase_##NAME(m_eglTestCtx));												\
	} while (deGetFalse())

#endif // _TEGLAPICASE_HPP
