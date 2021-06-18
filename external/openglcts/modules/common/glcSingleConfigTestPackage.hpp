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

class SingleConfigGL43TestPackage : public deqp::TestPackage
{
public:
	SingleConfigGL43TestPackage(tcu::TestContext& testCtx, const char* packageName,
							const char* description = "CTS Single Config GL43 Package",
							glu::ContextType renderContextType = glu::ContextType(glu::ApiType::core(4, 3)));
	~SingleConfigGL43TestPackage(void);

	void init(void);

	virtual tcu::TestCaseExecutor* createExecutor(void) const;

private:
	SingleConfigGL43TestPackage(const SingleConfigGL43TestPackage& other);
	SingleConfigGL43TestPackage& operator=(const SingleConfigGL43TestPackage& other);
};

class SingleConfigGL44TestPackage : public SingleConfigGL43TestPackage
{
public:
	SingleConfigGL44TestPackage(tcu::TestContext& testCtx, const char* packageName,
							const char* description = "CTS Single Config GL44 Package",
							glu::ContextType renderContextType = glu::ContextType(glu::ApiType::core(4, 4)));
	~SingleConfigGL44TestPackage(void);

	void init(void);

private:
	SingleConfigGL44TestPackage(const SingleConfigGL44TestPackage& other);
	SingleConfigGL44TestPackage& operator=(const SingleConfigGL44TestPackage& other);
};

class SingleConfigGL45TestPackage : public SingleConfigGL44TestPackage
{
public:
	SingleConfigGL45TestPackage(tcu::TestContext& testCtx, const char* packageName,
							const char* description = "CTS Single Config GL45 Package",
							glu::ContextType renderContextType = glu::ContextType(glu::ApiType::core(4, 5)));
	~SingleConfigGL45TestPackage(void);

	void init(void);

private:
	SingleConfigGL45TestPackage(const SingleConfigGL45TestPackage& other);
	SingleConfigGL45TestPackage& operator=(const SingleConfigGL45TestPackage& other);
};

class SingleConfigGL46TestPackage : public SingleConfigGL45TestPackage
{
public:
	SingleConfigGL46TestPackage(tcu::TestContext& testCtx, const char* packageName,
							const char* description = "CTS Single Config GL46 Package",
							glu::ContextType renderContextType = glu::ContextType(glu::ApiType::core(4, 6)));
	~SingleConfigGL46TestPackage(void);

	void init(void);

private:
	SingleConfigGL46TestPackage(const SingleConfigGL46TestPackage& other);
	SingleConfigGL46TestPackage& operator=(const SingleConfigGL46TestPackage& other);
};

class SingleConfigES32TestPackage : public deqp::TestPackage
{
public:
	SingleConfigES32TestPackage(tcu::TestContext& testCtx, const char* packageName,
							const char* description = "CTS Single Config ES32 Package",
							glu::ContextType renderContextType = glu::ContextType(glu::ApiType::es(3, 2)));
	~SingleConfigES32TestPackage(void);

	void init(void);

	virtual tcu::TestCaseExecutor* createExecutor(void) const;

private:
	SingleConfigES32TestPackage(const SingleConfigES32TestPackage& other);
	SingleConfigES32TestPackage& operator=(const SingleConfigES32TestPackage& other);
};


} // glcts

#endif // _GLCSINGLECONFIGTESTPACKAGE_HPP
