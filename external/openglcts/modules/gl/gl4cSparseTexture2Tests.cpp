/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
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
 * \brief
 */ /*-------------------------------------------------------------------*/

/**
 */ /*!
 * \file  gl4cSparseTexture2Tests.cpp
 * \brief Conformance tests for the GL_ARB_sparse_texture2 functionality.
 */ /*-------------------------------------------------------------------*/

#include "gl4cSparseTexture2Tests.hpp"
#include "gl4cSparseTextureTests.hpp"
#include "gluContextInfo.hpp"
#include "gluDefs.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuTestLog.hpp"

#include <cmath>
#include <string.h>
#include <vector>

using namespace glw;
using namespace glu;

namespace gl4cts
{

/** Constructor.
 *
 *  @param context     Rendering context
 */
StandardPageSizesTestCase::StandardPageSizesTestCase(deqp::Context& context)
	: TestCase(context, "StandardPageSizesTestCase",
			   "Verifies if values returned by GetInternalFormativ query matches Standard Virtual Page Sizes")
{
	/* Left blank intentionally */
}

/** Stub init method */
void StandardPageSizesTestCase::init()
{
	mSupportedTargets.push_back(GL_TEXTURE_1D);
	mSupportedTargets.push_back(GL_TEXTURE_1D_ARRAY);
	mSupportedTargets.push_back(GL_TEXTURE_2D);
	mSupportedTargets.push_back(GL_TEXTURE_2D_ARRAY);
	mSupportedTargets.push_back(GL_TEXTURE_CUBE_MAP);
	mSupportedTargets.push_back(GL_TEXTURE_CUBE_MAP_ARRAY);
	mSupportedTargets.push_back(GL_TEXTURE_RECTANGLE);
	mSupportedTargets.push_back(GL_TEXTURE_BUFFER);
	mSupportedTargets.push_back(GL_RENDERBUFFER);

	mStandardVirtualPageSizesTable[GL_R8]			  = PageSizeStruct(256, 256, 1);
	mStandardVirtualPageSizesTable[GL_R8_SNORM]		  = PageSizeStruct(256, 256, 1);
	mStandardVirtualPageSizesTable[GL_R8I]			  = PageSizeStruct(256, 256, 1);
	mStandardVirtualPageSizesTable[GL_R8UI]			  = PageSizeStruct(256, 256, 1);
	mStandardVirtualPageSizesTable[GL_R16]			  = PageSizeStruct(256, 128, 1);
	mStandardVirtualPageSizesTable[GL_R16_SNORM]	  = PageSizeStruct(256, 128, 1);
	mStandardVirtualPageSizesTable[GL_RG8]			  = PageSizeStruct(256, 128, 1);
	mStandardVirtualPageSizesTable[GL_RG8_SNORM]	  = PageSizeStruct(256, 128, 1);
	mStandardVirtualPageSizesTable[GL_RGB565]		  = PageSizeStruct(256, 128, 1);
	mStandardVirtualPageSizesTable[GL_R16F]			  = PageSizeStruct(256, 128, 1);
	mStandardVirtualPageSizesTable[GL_R16I]			  = PageSizeStruct(256, 128, 1);
	mStandardVirtualPageSizesTable[GL_R16UI]		  = PageSizeStruct(256, 128, 1);
	mStandardVirtualPageSizesTable[GL_RG8I]			  = PageSizeStruct(256, 128, 1);
	mStandardVirtualPageSizesTable[GL_RG8UI]		  = PageSizeStruct(256, 128, 1);
	mStandardVirtualPageSizesTable[GL_RG16]			  = PageSizeStruct(128, 128, 1);
	mStandardVirtualPageSizesTable[GL_RG16_SNORM]	 = PageSizeStruct(128, 128, 1);
	mStandardVirtualPageSizesTable[GL_RGBA8]		  = PageSizeStruct(128, 128, 1);
	mStandardVirtualPageSizesTable[GL_RGBA8_SNORM]	= PageSizeStruct(128, 128, 1);
	mStandardVirtualPageSizesTable[GL_RGB10_A2]		  = PageSizeStruct(128, 128, 1);
	mStandardVirtualPageSizesTable[GL_RGB10_A2UI]	 = PageSizeStruct(128, 128, 1);
	mStandardVirtualPageSizesTable[GL_RG16F]		  = PageSizeStruct(128, 128, 1);
	mStandardVirtualPageSizesTable[GL_R32F]			  = PageSizeStruct(128, 128, 1);
	mStandardVirtualPageSizesTable[GL_R11F_G11F_B10F] = PageSizeStruct(128, 128, 1);
	mStandardVirtualPageSizesTable[GL_RGB9_E5]		  = PageSizeStruct(128, 128, 1);
	mStandardVirtualPageSizesTable[GL_R32I]			  = PageSizeStruct(128, 128, 1);
	mStandardVirtualPageSizesTable[GL_R32UI]		  = PageSizeStruct(128, 128, 1);
	mStandardVirtualPageSizesTable[GL_RG16I]		  = PageSizeStruct(128, 128, 1);
	mStandardVirtualPageSizesTable[GL_RG16UI]		  = PageSizeStruct(128, 128, 1);
	mStandardVirtualPageSizesTable[GL_RGBA8I]		  = PageSizeStruct(128, 128, 1);
	mStandardVirtualPageSizesTable[GL_RGBA8UI]		  = PageSizeStruct(128, 128, 1);
	mStandardVirtualPageSizesTable[GL_RGBA16]		  = PageSizeStruct(128, 64, 1);
	mStandardVirtualPageSizesTable[GL_RGBA16_SNORM]   = PageSizeStruct(128, 64, 1);
	mStandardVirtualPageSizesTable[GL_RGBA16F]		  = PageSizeStruct(128, 64, 1);
	mStandardVirtualPageSizesTable[GL_RG32F]		  = PageSizeStruct(128, 64, 1);
	mStandardVirtualPageSizesTable[GL_RG32I]		  = PageSizeStruct(128, 64, 1);
	mStandardVirtualPageSizesTable[GL_RG32UI]		  = PageSizeStruct(128, 64, 1);
	mStandardVirtualPageSizesTable[GL_RGBA16I]		  = PageSizeStruct(128, 64, 1);
	mStandardVirtualPageSizesTable[GL_RGBA16UI]		  = PageSizeStruct(128, 64, 1);
	mStandardVirtualPageSizesTable[GL_RGBA32F]		  = PageSizeStruct(64, 64, 1);
	mStandardVirtualPageSizesTable[GL_RGBA32I]		  = PageSizeStruct(64, 64, 1);
	mStandardVirtualPageSizesTable[GL_RGBA32UI]		  = PageSizeStruct(64, 64, 1);
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult StandardPageSizesTestCase::iterate()
{
	const Functions& gl = m_context.getRenderContext().getFunctions();

	m_testCtx.getLog() << tcu::TestLog::Message << "Testing getInternalformativ" << tcu::TestLog::EndMessage;

	for (std::vector<glw::GLint>::const_iterator iter = mSupportedTargets.begin(); iter != mSupportedTargets.end();
		 ++iter)
	{
		const GLint& target = *iter;

		for (std::map<glw::GLint, PageSizeStruct>::const_iterator formIter = mStandardVirtualPageSizesTable.begin();
			 formIter != mStandardVirtualPageSizesTable.end(); ++formIter)
		{
			const PageSizePair&   format = *formIter;
			const PageSizeStruct& page   = format.second;

			GLint pageSizeX;
			GLint pageSizeY;
			GLint pageSizeZ;
			SparseTextureUtils::getTexturePageSizes(gl, target, format.first, pageSizeX, pageSizeY, pageSizeZ);

			if (pageSizeX != page.xSize || pageSizeY != page.ySize || pageSizeZ != page.zSize)
			{
				m_testCtx.getLog() << tcu::TestLog::Message << "Standard Virtual Page Size mismatch, target: " << target
								   << ", format: " << format.first << ", returned: " << pageSizeX << "/" << pageSizeY
								   << "/" << pageSizeZ << ", expected: " << page.xSize << "/" << page.ySize << "/"
								   << page.zSize << tcu::TestLog::EndMessage;

				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
				return STOP;
			}
		}
	}

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

	return STOP;
}

/** Constructor.
 *
 *  @param context     Rendering context
 */
SparseTexture2AllocationTestCase::SparseTexture2AllocationTestCase(deqp::Context& context)
	: SparseTextureAllocationTestCase(context, "SparseTexture2Allocation",
									  "Verifies TexStorage* functionality added in CTS_ARB_sparse_texture2")
{
	/* Left blank intentionally */
}

/** Initializes the test group contents. */
void SparseTexture2AllocationTestCase::init()
{
	mSupportedTargets.push_back(GL_TEXTURE_2D_MULTISAMPLE);
	mSupportedTargets.push_back(GL_TEXTURE_2D_MULTISAMPLE_ARRAY);

	mFullArrayTargets.push_back(GL_TEXTURE_2D_MULTISAMPLE_ARRAY);

	mSupportedInternalFormats.push_back(GL_R8);
	mSupportedInternalFormats.push_back(GL_R8_SNORM);
	mSupportedInternalFormats.push_back(GL_R16);
	mSupportedInternalFormats.push_back(GL_R16_SNORM);
	mSupportedInternalFormats.push_back(GL_RG8);
	mSupportedInternalFormats.push_back(GL_RG8_SNORM);
	mSupportedInternalFormats.push_back(GL_RG16);
	mSupportedInternalFormats.push_back(GL_RG16_SNORM);
	mSupportedInternalFormats.push_back(GL_RGB565);
	mSupportedInternalFormats.push_back(GL_RGBA8);
	mSupportedInternalFormats.push_back(GL_RGBA8_SNORM);
	mSupportedInternalFormats.push_back(GL_RGB10_A2);
	mSupportedInternalFormats.push_back(GL_RGB10_A2UI);
	mSupportedInternalFormats.push_back(GL_RGBA16);
	mSupportedInternalFormats.push_back(GL_RGBA16_SNORM);
	mSupportedInternalFormats.push_back(GL_R16F);
	mSupportedInternalFormats.push_back(GL_RG16F);
	mSupportedInternalFormats.push_back(GL_RGBA16F);
	mSupportedInternalFormats.push_back(GL_R32F);
	mSupportedInternalFormats.push_back(GL_RG32F);
	mSupportedInternalFormats.push_back(GL_RGBA32F);
	mSupportedInternalFormats.push_back(GL_R11F_G11F_B10F);
	mSupportedInternalFormats.push_back(GL_RGB9_E5);
	mSupportedInternalFormats.push_back(GL_R8I);
	mSupportedInternalFormats.push_back(GL_R8UI);
	mSupportedInternalFormats.push_back(GL_R16I);
	mSupportedInternalFormats.push_back(GL_R16UI);
	mSupportedInternalFormats.push_back(GL_R32I);
	mSupportedInternalFormats.push_back(GL_R32UI);
	mSupportedInternalFormats.push_back(GL_RG8I);
	mSupportedInternalFormats.push_back(GL_RG8UI);
	mSupportedInternalFormats.push_back(GL_RG16I);
	mSupportedInternalFormats.push_back(GL_RG16UI);
	mSupportedInternalFormats.push_back(GL_RG32I);
	mSupportedInternalFormats.push_back(GL_RG32UI);
	mSupportedInternalFormats.push_back(GL_RGBA8I);
	mSupportedInternalFormats.push_back(GL_RGBA8UI);
	mSupportedInternalFormats.push_back(GL_RGBA16I);
	mSupportedInternalFormats.push_back(GL_RGBA16UI);
	mSupportedInternalFormats.push_back(GL_RGBA32I);
}

/** Constructor.
 *
 *  @param context Rendering context.
 */
SparseTexture2Tests::SparseTexture2Tests(deqp::Context& context)
	: TestCaseGroup(context, "sparse_texture2_tests", "Verify conformance of CTS_ARB_sparse_texture2 implementation")
{
}

/** Initializes the test group contents. */
void SparseTexture2Tests::init()
{
	addChild(new StandardPageSizesTestCase(m_context));
	addChild(new SparseTextureAllocationTestCase(m_context));
}
} /* gl4cts namespace */
