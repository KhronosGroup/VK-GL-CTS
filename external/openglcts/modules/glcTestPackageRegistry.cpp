/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2016 Google Inc.
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief OpenGL Conformance Test Package Registry.
 */ /*-------------------------------------------------------------------*/

#include "glcTestPackageRegistry.hpp"
#include "glcConfigPackage.hpp"

#include "teglTestPackage.hpp"

#include "es2cTestPackage.hpp"
#include "tes2TestPackage.hpp"

#include "es3cTestPackage.hpp"
#include "tes3TestPackage.hpp"

#include "es31cTestPackage.hpp"
#include "esextcTestPackage.hpp"
#include "tes31TestPackage.hpp"
#include "tgl45es31TestPackage.hpp"
#include "tgl45es3TestPackage.hpp"

#include "es32cTestPackage.hpp"

#include "gl3cTestPackages.hpp"
#include "gl4cTestPackages.hpp"

#include "glcNoDefaultContextPackage.hpp"
#include "glcSingleConfigTestPackage.hpp"

namespace glcts
{

static tcu::TestPackage *createConfigPackage(tcu::TestContext &testCtx)
{
    return new glcts::ConfigPackage(testCtx, "CTS-Configs");
}

static tcu::TestPackage *createES2Package(tcu::TestContext &testCtx)
{
    return new es2cts::TestPackage(testCtx, "KHR-GLES2");
}

#if DE_OS != DE_OS_ANDROID
static tcu::TestPackage *createdEQPEGLPackage(tcu::TestContext &testCtx)
{
    return new deqp::egl::TestPackage(testCtx);
}
#endif

#if DE_OS != DE_OS_ANDROID
static tcu::TestPackage *createdEQPES2Package(tcu::TestContext &testCtx)
{
    return new deqp::gles2::TestPackage(testCtx);
}
#endif

static tcu::TestPackage *createES30Package(tcu::TestContext &testCtx)
{
    return new es3cts::ES30TestPackage(testCtx, "KHR-GLES3");
}

#if DE_OS != DE_OS_ANDROID
static tcu::TestPackage *createdEQPES30Package(tcu::TestContext &testCtx)
{
    return new deqp::gles3::TestPackage(testCtx);
}
#endif

#if DE_OS != DE_OS_ANDROID
static tcu::TestPackage *createdEQPES31Package(tcu::TestContext &testCtx)
{
    return new deqp::gles31::TestPackage(testCtx);
}
#endif

static tcu::TestPackage *createdEQPGL45ES31Package(tcu::TestContext &testCtx)
{
    return new deqp::gles31::TestPackageGL45ES31(testCtx);
}
static tcu::TestPackage *createdEQPGL45ES3Package(tcu::TestContext &testCtx)
{
    return new deqp::gles3::TestPackageGL45ES3(testCtx);
}
static tcu::TestPackage *createES31Package(tcu::TestContext &testCtx)
{
    return new es31cts::ES31TestPackage(testCtx, "KHR-GLES31");
}
static tcu::TestPackage *createESEXTPackage(tcu::TestContext &testCtx)
{
    return new esextcts::ESEXTTestPackage(testCtx, "KHR-GLESEXT");
}

static tcu::TestPackage *createES32Package(tcu::TestContext &testCtx)
{
    return new es32cts::ES32TestPackage(testCtx, "KHR-GLES32");
}

static tcu::TestPackage *createNoDefaultCustomContextPackage(tcu::TestContext &testCtx)
{
    return new glcts::NoDefaultContextPackage(testCtx, "KHR-NoContext");
}
static tcu::TestPackage *createSingleConfigGL43TestPackage(tcu::TestContext &testCtx)
{
    return new glcts::SingleConfigGL43TestPackage(testCtx, "KHR-Single-GL43");
}
static tcu::TestPackage *createSingleConfigGL44TestPackage(tcu::TestContext &testCtx)
{
    return new glcts::SingleConfigGL44TestPackage(testCtx, "KHR-Single-GL44");
}
static tcu::TestPackage *createSingleConfigGL45TestPackage(tcu::TestContext &testCtx)
{
    return new glcts::SingleConfigGL45TestPackage(testCtx, "KHR-Single-GL45");
}
static tcu::TestPackage *createSingleConfigGL46TestPackage(tcu::TestContext &testCtx)
{
    return new glcts::SingleConfigGL45TestPackage(testCtx, "KHR-Single-GL46");
}
static tcu::TestPackage *createSingleConfigES32TestPackage(tcu::TestContext &testCtx)
{
    return new glcts::SingleConfigES32TestPackage(testCtx, "KHR-Single-GLES32");
}

static tcu::TestPackage *createGL30Package(tcu::TestContext &testCtx)
{
    return new gl3cts::GL30TestPackage(testCtx, "KHR-GL30");
}
static tcu::TestPackage *createGL31Package(tcu::TestContext &testCtx)
{
    return new gl3cts::GL31TestPackage(testCtx, "KHR-GL31");
}
static tcu::TestPackage *createGL32Package(tcu::TestContext &testCtx)
{
    return new gl3cts::GL32TestPackage(testCtx, "KHR-GL32");
}
static tcu::TestPackage *createGL33Package(tcu::TestContext &testCtx)
{
    return new gl3cts::GL33TestPackage(testCtx, "KHR-GL33");
}

static tcu::TestPackage *createGL40Package(tcu::TestContext &testCtx)
{
    return new gl4cts::GL40TestPackage(testCtx, "KHR-GL40");
}
static tcu::TestPackage *createGL41Package(tcu::TestContext &testCtx)
{
    return new gl4cts::GL41TestPackage(testCtx, "KHR-GL41");
}
static tcu::TestPackage *createGL42Package(tcu::TestContext &testCtx)
{
    return new gl4cts::GL42TestPackage(testCtx, "KHR-GL42");
}
static tcu::TestPackage *createGL42CompatPackage(tcu::TestContext &testCtx)
{
    return new gl4cts::GL42CompatTestPackage(testCtx, "KHR-GL42-COMPAT");
}
static tcu::TestPackage *createGL43Package(tcu::TestContext &testCtx)
{
    return new gl4cts::GL43TestPackage(testCtx, "KHR-GL43");
}
static tcu::TestPackage *createGL44Package(tcu::TestContext &testCtx)
{
    return new gl4cts::GL44TestPackage(testCtx, "KHR-GL44");
}
static tcu::TestPackage *createGL45Package(tcu::TestContext &testCtx)
{
    return new gl4cts::GL45TestPackage(testCtx, "KHR-GL45");
}
static tcu::TestPackage *createGL46Package(tcu::TestContext &testCtx)
{
    return new gl4cts::GL46TestPackage(testCtx, "KHR-GL46");
}

void registerPackages(void)
{
    tcu::TestPackageRegistry *registry = tcu::TestPackageRegistry::getSingleton();

    registry->registerPackage("CTS-Configs", createConfigPackage);

#if DE_OS != DE_OS_ANDROID
    registry->registerPackage("dEQP-EGL", createdEQPEGLPackage);
#endif
    registry->registerPackage("KHR-GLES2", createES2Package);
#if DE_OS != DE_OS_ANDROID
    registry->registerPackage("dEQP-GLES2", createdEQPES2Package);
#endif

    registry->registerPackage("KHR-GLES3", createES30Package);
#if DE_OS != DE_OS_ANDROID
    registry->registerPackage("dEQP-GLES3", createdEQPES30Package);
#endif

#if DE_OS != DE_OS_ANDROID
    registry->registerPackage("dEQP-GLES31", createdEQPES31Package);
#endif
    registry->registerPackage("dEQP-GL45-GLES31", createdEQPGL45ES31Package);
    registry->registerPackage("dEQP-GL45-GLES3", createdEQPGL45ES3Package);
    registry->registerPackage("KHR-GLES31", createES31Package);
    registry->registerPackage("KHR-GLESEXT", createESEXTPackage);

    registry->registerPackage("KHR-GLES32", createES32Package);

    registry->registerPackage("KHR-NoContext", createNoDefaultCustomContextPackage);
    registry->registerPackage("KHR-Single-GL43", createSingleConfigGL43TestPackage);
    registry->registerPackage("KHR-Single-GL44", createSingleConfigGL44TestPackage);
    registry->registerPackage("KHR-Single-GL45", createSingleConfigGL45TestPackage);
    registry->registerPackage("KHR-Single-GL46", createSingleConfigGL46TestPackage);
    registry->registerPackage("KHR-Single-GLES32", createSingleConfigES32TestPackage);

    registry->registerPackage("KHR-GL30", createGL30Package);
    registry->registerPackage("KHR-GL31", createGL31Package);
    registry->registerPackage("KHR-GL32", createGL32Package);
    registry->registerPackage("KHR-GL33", createGL33Package);

    registry->registerPackage("KHR-GL40", createGL40Package);
    registry->registerPackage("KHR-GL41", createGL41Package);
    registry->registerPackage("KHR-GL42", createGL42Package);
    registry->registerPackage("KHR-COMPAT-GL42", createGL42CompatPackage);
    registry->registerPackage("KHR-GL43", createGL43Package);
    registry->registerPackage("KHR-GL44", createGL44Package);
    registry->registerPackage("KHR-GL45", createGL45Package);
    registry->registerPackage("KHR-GL46", createGL46Package);
}
} // namespace glcts
