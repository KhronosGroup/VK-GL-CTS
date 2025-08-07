/*
 * Copyright (c) 2022 Shenzhen Kaihong Digital Industry Development Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdio>
#include <iostream>
#include "tcuDefs.hpp"
#include "tcuCommandLine.hpp"
#include "tcuPlatform.hpp"
#include "tcuApp.hpp"
#include "tcuResource.hpp"
#include "tcuTestLog.hpp"
#include "tcuTestSessionExecutor.hpp"
#include "deUniquePtr.hpp"

#include "common/glcConfigPackage.hpp"
#include "common/glcTestPackage.hpp"
#include "gles2/es2cTestPackage.hpp"
#include "gles3/es3cTestPackage.hpp"
#include "gles32/es32cTestPackage.hpp"
#include "gles31/es31cTestPackage.hpp"

#include "gles2/tes2TestPackage.hpp"
#include "gles3/tes3TestPackage.hpp"
#include "gles31/tes31TestPackage.hpp"

#include "ohos_context_i.h"

#include "tcuTestContext.hpp"
#include "tcuOhosPlatform.hpp"

// #undef LOG_TAG
// #define LOG_TAG "vkglcts"

// #undef LOG_DOMAIN
// #define  LOG_DOMAIN    0xD001560

// #define LOGD(...) ((void)HiLogPrint(LOG_CORE, LOG_DEBUG, LOG_DOMAIN, LOG_TAG, __VA_ARGS__))

// #define LOGI(...) ((void)HiLogPrint(LOG_CORE, LOG_INFO, LOG_DOMAIN, LOG_TAG, __VA_ARGS__))

// #define LOGW(...) ((void)HiLogPrint(LOG_CORE, LOG_WARN, LOG_DOMAIN, LOG_TAG, __VA_ARGS__))

// #define LOGE(...) ((void)HiLogPrint(LOG_CORE, LOG_ERROR, LOG_DOMAIN, LOG_TAG, __VA_ARGS__))

// #define LOGF(...) ((void)HiLogPrint(LOG_CORE, LOG_FATAL, LOG_DOMAIN, LOG_TAG, __VA_ARGS__))

tcu::Platform *createPlatform(void);

static tcu::TestPackage *createES2Package(tcu::TestContext &testCtx)
{
    return new es2cts::TestPackage(testCtx, "KHR-GLES2");
}

static tcu::TestPackage *createES32Package(tcu::TestContext &testCtx)
{
    return new es32cts::ES32TestPackage(testCtx, "KHR-GLES32");
}
static tcu::TestPackage* createES30Package(tcu::TestContext& testCtx)
{
	return new es3cts::ES30TestPackage(testCtx, "KHR-GLES3");
}
static tcu::TestPackage* createES31Package(tcu::TestContext& testCtx)
{
	return new es31cts::ES31TestPackage(testCtx, "KHR-GLES31");
}

static tcu::TestPackage* createdEQPES2Package(tcu::TestContext& testCtx)
{
	return new deqp::gles2::TestPackage(testCtx);
}
static tcu::TestPackage* createdEQPES30Package(tcu::TestContext& testCtx)
{
	return new deqp::gles3::TestPackage(testCtx);
}
static tcu::TestPackage* createdEQPES31Package(tcu::TestContext& testCtx)
{
	return new deqp::gles31::TestPackage(testCtx);
}

// Implement this in your platform port.

void RegistPackage()
{

    tcu::TestPackageRegistry *registry = tcu::TestPackageRegistry::getSingleton();
    // registry->registerPackage("CTS-Configs", createConfigPackage);

    // TODO: 判断当前上下文EGL环境是哪个?
    /*
KHR-GLES2
KHR-GLES3
KHR-GLES31
KHR-GLESEXT
KHR-GLES32
    */
    // OHOS::Rosen::RosenContext::GetInstance().GetGlesVer() == 3.2
    registry->registerPackage("KHR-GLES31", createES31Package);
    registry->registerPackage("KHR-GLES2", createES2Package);
    registry->registerPackage("KHR-GLES3", createES30Package);
    registry->registerPackage("KHR-GLES32", createES32Package);
    registry->registerPackage("dEQP-GLES2", createdEQPES2Package);
    registry->registerPackage("dEQP-GLES3", createdEQPES30Package);
    registry->registerPackage("dEQP-GLES31", createdEQPES31Package);
}

bool GetCasePath(tcu::TestNode *node, std::vector<tcu::TestNode *> &casePath, std::vector<std::string> &namePath, uint32_t deep = 0)
{
    if (deep >= namePath.size())
        return false;
    if (namePath[deep].compare(node->getName()) != 0)
        return false;
    casePath.push_back(node);
    switch (node->getNodeType())
    {
    case tcu::NODETYPE_ROOT: // = 0,		//!< Root for all test packages.
        printf("NODETYPE_ROOT\n");
        break;
    case tcu::NODETYPE_PACKAGE: //,		//!< Test case package -- same as group, but is omitted from XML dump.
    case tcu::NODETYPE_GROUP: //,			//!< Test case container -- cannot be executed.
        printf("NODETYPE_GROUP\n");
        {
            std::vector<tcu::TestNode *> children;
            node->getChildren(children);
            for (uint32_t i = 0; i < children.size(); i++)
            {
                // printf("-----------%s==%s\n",children[i]->getName(),namePath[deep+1].c_str());
                if (GetCasePath(children[i], casePath, namePath, deep + 1))
                    return true;
            }
        }
        break;
    case tcu::NODETYPE_SELF_VALIDATE: //,	//!< Self-validating test case -- can be executed
        printf("NODETYPE_SELF_VALIDATE\n");
        return true;
    case tcu::NODETYPE_PERFORMANCE: //,	//!< Performace test case -- can be executed
        printf("NODETYPE_PERFORMANCE\n");
        return true;
    case tcu::NODETYPE_CAPABILITY: //,	//!< Capability score case -- can be executed
        printf("NODETYPE_CAPABILITY\n");
        return true;
    case tcu::NODETYPE_ACCURACY: //		//!< Accuracy test case -- can be executed
        printf("NODETYPE_ACCURACY\n");
        return true;
    }
    return false;
}

static int isInit = 0;



typedef struct TestRunStatus
{
	int		numExecuted;		//!< Total number of cases executed.
	int		numPassed;			//!< Number of cases passed.
	int		numFailed;			//!< Number of cases failed.
	int		numNotSupported;	//!< Number of cases not supported.
	int		numWarnings;		//!< Number of QualityWarning / CompatibilityWarning results.
	int		numWaived;			//!< Number of waived tests.
	bool	isComplete;			//!< Is run complete.
} TestRunStatus_t;

__attribute__((visibility("default"))) TestRunStatus_t main1(int argc, char **argv);

TestRunStatus_t main1(int argc, char **argv)
{
    printf("start test main--- \n");
    int exitStatus = EXIT_SUCCESS;
    TestRunStatus_t runResult;

#if (DE_OS != DE_OS_WIN32)
    // Set stdout to line-buffered mode (will be fully buffered by default if stdout is pipe).
    setvbuf(stdout, nullptr, _IOLBF, 4 * 1024);
#endif

    try
    {
        if (isInit == 0) {
            RegistPackage();
            isInit = 1;
        }
        
        tcu::CommandLine cmdLine(argc, argv);
        tcu::DirArchive archive(cmdLine.getArchiveDir());
        tcu::TestLog log(cmdLine.getLogFileName(), cmdLine.getLogFlags());
        de::UniquePtr<tcu::Platform> platform(createOhosPlatform());
        de::UniquePtr<tcu::App> app(new tcu::App(*platform, archive, log, cmdLine));

        // Main loop.
        for (;;)
        {
            if (!app->iterate())
            {
                if (cmdLine.getRunMode() == tcu::RUNMODE_EXECUTE &&
                    (!app->getResult().isComplete || app->getResult().numFailed))
                {
                    exitStatus = EXIT_FAILURE;
                }

                break;
            }
        }

        tcu::TestRunStatus trs = app->getResult();
        printf("finish test main--- pass:%{public}d, fail:%{public}d, all:%{public}d\n", 
            trs.numPassed, trs.numFailed, trs.numExecuted);
        
        runResult.isComplete = trs.isComplete;
        runResult.numExecuted = trs.numExecuted;
        runResult.numPassed = trs.numPassed;
        runResult.numFailed = trs.numFailed;		
        runResult.numNotSupported = trs.numNotSupported;
        runResult.numWarnings = trs.numWarnings;
        runResult.numWaived = trs.numWaived;
        runResult.isComplete = trs.isComplete;
        printf("before return--- pass:%{public}d, fail:%{public}d, all:%{public}d\n", 
            runResult.numPassed, runResult.numFailed, runResult.numExecuted);
        return runResult;
        
    }
    catch (const std::exception &e)
    {
        printf("catch error : %s", e.what());
        tcu::die("%s", e.what());
    }

    return runResult;
}