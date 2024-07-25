#ifndef _VKTTESTPACKAGE_HPP
#define _VKTTESTPACKAGE_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Vulkan Test Package
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestPackage.hpp"
#include "tcuResource.hpp"
#include "vktTestCase.hpp"

namespace vkt
{

class BaseTestPackage : public tcu::TestPackage
{
public:
    BaseTestPackage(tcu::TestContext &testCtx, const char *name);
    virtual ~BaseTestPackage(void);

    tcu::TestCaseExecutor *createExecutor(void) const;
};

#ifdef CTS_USES_VULKAN

class TestPackage : public BaseTestPackage
{
public:
    TestPackage(tcu::TestContext &testCtx);
    virtual ~TestPackage(void);

    virtual void init(void);
};

class ExperimentalTestPackage : public BaseTestPackage
{
public:
    ExperimentalTestPackage(tcu::TestContext &testCtx);
    virtual ~ExperimentalTestPackage(void);

    virtual void init(void);
};

#endif

#ifdef CTS_USES_VULKANSC

class TestPackageSC : public BaseTestPackage
{
public:
    TestPackageSC(tcu::TestContext &testCtx);
    virtual ~TestPackageSC(void);

    virtual void init(void);
};

#endif // CTS_USES_VULKANSC

} // namespace vkt

#endif // _VKTTESTPACKAGE_HPP
