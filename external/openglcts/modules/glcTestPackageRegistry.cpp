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

#include "es2cTestPackage.hpp"
#include "tes2TestPackage.hpp"

#if defined(DEQP_GTF_AVAILABLE)
#include "gtfES2TestPackage.hpp"
#endif

#include "es3cTestPackage.hpp"
#include "tes3TestPackage.hpp"

#if defined(DEQP_GTF_AVAILABLE)
#include "gtfES3TestPackage.hpp"
#endif

#include "es31cTestPackage.hpp"
#include "esextcTestPackage.hpp"
#include "tes31TestPackage.hpp"

#if defined(DEQP_GTF_AVAILABLE)
#include "gtfES31TestPackage.hpp"
#endif

#include "es32cTestPackage.hpp"

#include "gl3cTestPackages.hpp"
#include "gl4cTestPackages.hpp"

#if defined(DEQP_GTF_AVAILABLE)
#include "gtfGL30TestPackage.hpp"
#include "gtfGL31TestPackage.hpp"
#include "gtfGL32TestPackage.hpp"
#include "gtfGL33TestPackage.hpp"
#include "gtfGL40TestPackage.hpp"
#include "gtfGL41TestPackage.hpp"
#include "gtfGL42TestPackage.hpp"
#include "gtfGL43TestPackage.hpp"
#include "gtfGL44TestPackage.hpp"
#include "gtfGL45TestPackage.hpp"
#endif

namespace glcts
{

static tcu::TestPackage* createConfigPackage(tcu::TestContext& testCtx)
{
	return new glcts::ConfigPackage(testCtx, "CTS-Configs");
}

static tcu::TestPackage* createES2Package(tcu::TestContext& testCtx)
{
	return new es2cts::TestPackage(testCtx, "KHR-GLES2");
}
static tcu::TestPackage* createdEQPES2Package(tcu::TestContext& testCtx)
{
	return new deqp::gles2::TestPackage(testCtx);
}

#if defined(DEQP_GTF_AVAILABLE)
static tcu::TestPackage* createES2GTFPackage(tcu::TestContext& testCtx)
{
	return new gtf::es2::TestPackage(testCtx, "GTF-GLES2");
}
#endif

static tcu::TestPackage* createES30Package(tcu::TestContext& testCtx)
{
	return new es3cts::ES30TestPackage(testCtx, "KHR-GLES3");
}
static tcu::TestPackage* createdEQPES30Package(tcu::TestContext& testCtx)
{
	return new deqp::gles3::TestPackage(testCtx);
}

#if defined(DEQP_GTF_AVAILABLE)
static tcu::TestPackage* createES30GTFPackage(tcu::TestContext& testCtx)
{
	return new gtf::es3::TestPackage(testCtx, "GTF-GLES3");
}
#endif

static tcu::TestPackage* createdEQPES31Package(tcu::TestContext& testCtx)
{
	return new deqp::gles31::TestPackage(testCtx);
}
static tcu::TestPackage* createES31Package(tcu::TestContext& testCtx)
{
	return new es31cts::ES31TestPackage(testCtx, "KHR-GLES31");
}
static tcu::TestPackage* createESEXTPackage(tcu::TestContext& testCtx)
{
	return new esextcts::ESEXTTestPackage(testCtx, "KHR-GLESEXT");
}

#if defined(DEQP_GTF_AVAILABLE)
static tcu::TestPackage* createES31GTFPackage(tcu::TestContext& testCtx)
{
	return new gtf::es31::TestPackage(testCtx, "GTF-GLES31");
}
#endif

static tcu::TestPackage* createES32Package(tcu::TestContext& testCtx)
{
	return new es32cts::ES32TestPackage(testCtx, "KHR-GLES32");
}

static tcu::TestPackage* createGL30Package(tcu::TestContext& testCtx)
{
	return new gl3cts::GL30TestPackage(testCtx, "GL30-CTS");
}
static tcu::TestPackage* createGL31Package(tcu::TestContext& testCtx)
{
	return new gl3cts::GL31TestPackage(testCtx, "GL31-CTS");
}
static tcu::TestPackage* createGL32Package(tcu::TestContext& testCtx)
{
	return new gl3cts::GL32TestPackage(testCtx, "GL32-CTS");
}
static tcu::TestPackage* createGL33Package(tcu::TestContext& testCtx)
{
	return new gl3cts::GL33TestPackage(testCtx, "GL33-CTS");
}

static tcu::TestPackage* createGL40Package(tcu::TestContext& testCtx)
{
	return new gl4cts::GL40TestPackage(testCtx, "GL40-CTS");
}
static tcu::TestPackage* createGL41Package(tcu::TestContext& testCtx)
{
	return new gl4cts::GL41TestPackage(testCtx, "GL41-CTS");
}
static tcu::TestPackage* createGL42Package(tcu::TestContext& testCtx)
{
	return new gl4cts::GL42TestPackage(testCtx, "GL42-CTS");
}
static tcu::TestPackage* createGL43Package(tcu::TestContext& testCtx)
{
	return new gl4cts::GL43TestPackage(testCtx, "GL43-CTS");
}
static tcu::TestPackage* createGL44Package(tcu::TestContext& testCtx)
{
	return new gl4cts::GL44TestPackage(testCtx, "GL44-CTS");
}
static tcu::TestPackage* createGL45Package(tcu::TestContext& testCtx)
{
	return new gl4cts::GL45TestPackage(testCtx, "GL45-CTS");
}

#if defined(DEQP_GTF_AVAILABLE)
static tcu::TestPackage* createGL30GTFPackage(tcu::TestContext& testCtx)
{
	return new gtf::gl30::TestPackage(testCtx, "GL30-GTF");
}
static tcu::TestPackage* createGL31GTFPackage(tcu::TestContext& testCtx)
{
	return new gtf::gl31::TestPackage(testCtx, "GL31-GTF");
}
static tcu::TestPackage* createGL32GTFPackage(tcu::TestContext& testCtx)
{
	return new gtf::gl32::TestPackage(testCtx, "GL32-GTF");
}
static tcu::TestPackage* createGL33GTFPackage(tcu::TestContext& testCtx)
{
	return new gtf::gl32::TestPackage(testCtx, "GL33-GTF");
}

static tcu::TestPackage* createGL40GTFPackage(tcu::TestContext& testCtx)
{
	return new gtf::gl40::TestPackage(testCtx, "GL40-GTF");
}
static tcu::TestPackage* createGL41GTFPackage(tcu::TestContext& testCtx)
{
	return new gtf::gl41::TestPackage(testCtx, "GL41-GTF");
}
static tcu::TestPackage* createGL42GTFPackage(tcu::TestContext& testCtx)
{
	return new gtf::gl42::TestPackage(testCtx, "GL42-GTF");
}
static tcu::TestPackage* createGL43GTFPackage(tcu::TestContext& testCtx)
{
	return new gtf::gl43::TestPackage(testCtx, "GL43-GTF");
}
static tcu::TestPackage* createGL44GTFPackage(tcu::TestContext& testCtx)
{
	return new gtf::gl44::TestPackage(testCtx, "GL44-GTF");
}
static tcu::TestPackage* createGL45GTFPackage(tcu::TestContext& testCtx)
{
	return new gtf::gl45::TestPackage(testCtx, "GL45-GTF");
}
#endif

void registerPackages(void)
{
	tcu::TestPackageRegistry* registry = tcu::TestPackageRegistry::getSingleton();

	registry->registerPackage("CTS-Configs", createConfigPackage);

	registry->registerPackage("KHR-GLES2", createES2Package);
	registry->registerPackage("dEQP-GLES2", createdEQPES2Package);

#if defined(DEQP_GTF_AVAILABLE)
	registry->registerPackage("GTF-GLES2", createES2GTFPackage);
#endif

	registry->registerPackage("KHR-GLES3", createES30Package);
	registry->registerPackage("dEQP-GLES3", createdEQPES30Package);

#if defined(DEQP_GTF_AVAILABLE)
	registry->registerPackage("GTF-GLES3", createES30GTFPackage);
#endif

	registry->registerPackage("dEQP-GLES31", createdEQPES31Package);
	registry->registerPackage("KHR-GLES31", createES31Package);
	registry->registerPackage("KHR-GLESEXT", createESEXTPackage);

#if defined(DEQP_GTF_AVAILABLE)
	registry->registerPackage("GTF-GLES31", createES31GTFPackage);
#endif

	registry->registerPackage("KHR-GLES32", createES32Package);

	registry->registerPackage("GL30-CTS", createGL30Package);
	registry->registerPackage("GL31-CTS", createGL31Package);
	registry->registerPackage("GL32-CTS", createGL32Package);
	registry->registerPackage("GL33-CTS", createGL33Package);

	registry->registerPackage("GL40-CTS", createGL40Package);
	registry->registerPackage("GL41-CTS", createGL41Package);
	registry->registerPackage("GL42-CTS", createGL42Package);
	registry->registerPackage("GL43-CTS", createGL43Package);
	registry->registerPackage("GL44-CTS", createGL44Package);
	registry->registerPackage("GL45-CTS", createGL45Package);

#if defined(DEQP_GTF_AVAILABLE)
	registry->registerPackage("GL30-GTF", createGL30GTFPackage);
	registry->registerPackage("GL31-GTF", createGL31GTFPackage);
	registry->registerPackage("GL32-GTF", createGL32GTFPackage);
	registry->registerPackage("GL33-GTF", createGL33GTFPackage);

	registry->registerPackage("GL40-GTF", createGL40GTFPackage);
	registry->registerPackage("GL41-GTF", createGL41GTFPackage);
	registry->registerPackage("GL42-GTF", createGL42GTFPackage);
	registry->registerPackage("GL43-GTF", createGL43GTFPackage);
	registry->registerPackage("GL44-GTF", createGL44GTFPackage);
	registry->registerPackage("GL45-GTF", createGL45GTFPackage);
#endif
}
}
