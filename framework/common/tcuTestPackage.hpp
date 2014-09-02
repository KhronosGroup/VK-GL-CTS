#ifndef _TCUTESTPACKAGE_HPP
#define _TCUTESTPACKAGE_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief Base class for a test case.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "tcuTestCaseWrapper.hpp"

namespace tcu
{

/*--------------------------------------------------------------------*//*!
 * \brief Base class for test packages.
 *
 * Test packages are root-level test groups. Test case exposes couple of
 * extra customization points. Test package can define custom TestCaseWrapper
 * and archive (usually ResourcePrefix around default archive) for resources.
 *
 * Test package is typically responsible of setting up rendering context
 * for test cases.
 *//*--------------------------------------------------------------------*/
class TestPackage : public TestNode
{
public:
								TestPackage			(TestContext& testCtx, const char* name, const char* description);
	virtual						~TestPackage		(void);

	virtual IterateResult		iterate				(void);

	virtual TestCaseWrapper&	getTestCaseWrapper	(void) = DE_NULL;
	virtual Archive&			getArchive			(void) = DE_NULL;
};

// TestPackageRegistry

typedef TestPackage* (*TestPackageCreateFunc)	(TestContext& testCtx);

class TestPackageRegistry
{
public:
	struct PackageInfo
	{
		PackageInfo (std::string name_, TestPackageCreateFunc createFunc_) : name(name_), createFunc(createFunc_) {}

		std::string				name;
		TestPackageCreateFunc	createFunc;
	};

	static TestPackageRegistry*			getSingleton			(void);
	static void							destroy					(void);

	void								registerPackage			(const char* name, TestPackageCreateFunc createFunc);
	const std::vector<PackageInfo*>&	getPackageInfos			(void) const;
	PackageInfo*						getPackageInfoByName	(const char* name) const;
	TestPackage*						createPackage			(const char* name, TestContext& testCtx) const;

private:
										TestPackageRegistry		(void);
										~TestPackageRegistry	(void);

	static TestPackageRegistry*			getOrDestroy			(bool isCreate);

	// Member variables.
	std::vector<PackageInfo*>			m_packageInfos;
};

// TestPackageDescriptor

class TestPackageDescriptor
{
public:
						TestPackageDescriptor		(const char* name, TestPackageCreateFunc createFunc);
						~TestPackageDescriptor		(void);
};

// TestPackageRoot

class TestPackageRoot : public TestNode
{
public:
							TestPackageRoot		(TestContext& testCtx);
							TestPackageRoot		(TestContext& testCtx, const std::vector<TestNode*>& children);
	virtual					~TestPackageRoot	(void);

	virtual IterateResult	iterate				(void);
};

} // tcu

#endif // _TCUTESTPACKAGE_HPP
