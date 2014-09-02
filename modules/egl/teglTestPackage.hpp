#ifndef _TEGLTESTPACKAGE_HPP
#define _TEGLTESTPACKAGE_HPP
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
 * \brief EGL Test Package
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestPackage.hpp"
#include "teglTestCase.hpp"
#include "tcuResource.hpp"

namespace deqp
{
namespace egl
{

class TestCaseWrapper : public tcu::TestCaseWrapper
{
public:
									TestCaseWrapper			(EglTestContext& eglTestContext);
									~TestCaseWrapper		(void);

	bool							initTestCase			(tcu::TestCase* testCase);
	bool							deinitTestCase			(tcu::TestCase* testCase);

	tcu::TestNode::IterateResult	iterateTestCase			(tcu::TestCase* testCase);

private:
	EglTestContext&					m_eglTestCtx;
};

class PackageContext
{
public:
									PackageContext			(tcu::TestContext& testCtx);
									~PackageContext			(void);

	EglTestContext&					getEglTestContext		(void) { return *m_eglTestCtx;	}
	tcu::TestCaseWrapper&			getTestCaseWrapper		(void) { return *m_caseWrapper;	}

private:
	EglTestContext*					m_eglTestCtx;
	TestCaseWrapper*				m_caseWrapper;
};

class TestPackage : public tcu::TestPackage
{
public:
									TestPackage				(tcu::TestContext& testCtx);
	virtual							~TestPackage			(void);

	virtual void					init					(void);
	virtual void					deinit					(void);

	tcu::TestCaseWrapper&			getTestCaseWrapper		(void) { return m_packageCtx->getTestCaseWrapper(); }
	tcu::ResourcePrefix&			getArchive				(void) { return m_archive; }

private:
	PackageContext*					m_packageCtx;
	tcu::ResourcePrefix				m_archive;
};

} // egl
} // deqp

#endif // _TEGLTESTPACKAGE_HPP
