/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Copyright (c) 2016 The Android Open Source Project
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
 * \brief Shader builtin variable tests.
 *//*--------------------------------------------------------------------*/

#include "vktShaderRenderBuiltinVarTests.hpp"
#include "vktShaderRender.hpp"
#include "gluShaderUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTextureUtil.hpp"

using namespace std;
using namespace tcu;
using namespace vk;

namespace vkt
{
namespace sr
{

namespace
{

class BuiltinGlFrontFacingCaseInstance : public ShaderRenderCaseInstance
{
public:
					BuiltinGlFrontFacingCaseInstance	(Context& context);

	TestStatus		iterate								(void);
	virtual void	setupDefaultInputs					(void);
};

BuiltinGlFrontFacingCaseInstance::BuiltinGlFrontFacingCaseInstance (Context& context)
	: ShaderRenderCaseInstance	(context)
{
}

TestStatus BuiltinGlFrontFacingCaseInstance::iterate (void)
{
	const UVec2		viewportSize	= getViewportSize();
	const int		width			= viewportSize.x();
	const int		height			= viewportSize.y();
	const RGBA		threshold		(2, 2, 2, 2);
	Surface			resImage		(width, height);
	Surface			refImage		(width, height);
	bool			compareOk		= false;
	const deUint16	indices[12]		=
	{
		0, 4, 1,
		0, 5, 4,
		1, 2, 3,
		1, 3, 4
	};

	setup();
	render(6, 4, indices);
	copy(resImage.getAccess(), getResultImage().getAccess());

	for (int y = 0; y < refImage.getHeight(); y++)
	{
		for (int x = 0; x < refImage.getWidth()/2; x++)
			refImage.setPixel(x, y, RGBA::green());

		for (int x = refImage.getWidth()/2; x < refImage.getWidth(); x++)
			refImage.setPixel(x, y, RGBA::blue());
	}

	compareOk = pixelThresholdCompare(m_context.getTestContext().getLog(), "Result", "Image comparison result", refImage, resImage, threshold, COMPARE_LOG_RESULT);

	if (compareOk)
		return TestStatus::pass("Result image matches reference");
	else
		return TestStatus::fail("Image mismatch");
}

void BuiltinGlFrontFacingCaseInstance::setupDefaultInputs (void)
{
	const float vertices[] =
	{
		-1.0f, -1.0f, 0.0f, 1.0f,
		 0.0f, -1.0f, 0.0f, 1.0f,
		 1.0f, -1.0f, 0.0f, 1.0f,
		 1.0f,  1.0f, 0.0f, 1.0f,
		 0.0f,  1.0f, 0.0f, 1.0f,
		-1.0f,  1.0f, 0.0f, 1.0f
	};

	addAttribute(0u, VK_FORMAT_R32G32B32A32_SFLOAT, (deUint32)sizeof(float) * 4, 6, vertices);
}

class BuiltinGlFrontFacingCase : public TestCase
{
public:
								BuiltinGlFrontFacingCase	(TestContext& testCtx, const string& name, const string& description);
	virtual						~BuiltinGlFrontFacingCase	(void);

	void						initPrograms				(SourceCollections& dst) const;
	TestInstance*				createInstance				(Context& context) const;

private:
								BuiltinGlFrontFacingCase	(const BuiltinGlFrontFacingCase&);	// not allowed!
	BuiltinGlFrontFacingCase&	operator=					(const BuiltinGlFrontFacingCase&);	// not allowed!
};

BuiltinGlFrontFacingCase::BuiltinGlFrontFacingCase (TestContext& testCtx, const string& name, const string& description)
	: TestCase(testCtx, name, description)
{
}

BuiltinGlFrontFacingCase::~BuiltinGlFrontFacingCase (void)
{
}

void BuiltinGlFrontFacingCase::initPrograms (SourceCollections& dst) const
{
	dst.glslSources.add("vert") << glu::VertexSource(
		"#version 310 es\n"
		"layout(location = 0) in highp vec4 a_position;\n"
		"void main (void)\n"
		"{\n"
		"       gl_Position = a_position;\n"
		"}\n");

	dst.glslSources.add("frag") << glu::FragmentSource(
		"#version 310 es\n"
		"layout(location = 0) out lowp vec4 o_color;\n"
		"void main (void)\n"
		"{\n"
		"       if (gl_FrontFacing)\n"
		"               o_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
		"       else\n"
		"               o_color = vec4(0.0, 0.0, 1.0, 1.0);\n"
		"}\n");
}

TestInstance* BuiltinGlFrontFacingCase::createInstance (Context& context) const
{
	return new BuiltinGlFrontFacingCaseInstance(context);
}

} // anonymous

TestCaseGroup* createBuiltinVarTests (TestContext& testCtx)
{
	de::MovePtr<TestCaseGroup> varyingGroup(new TestCaseGroup(testCtx, "builtin_var", "Shader builtin variable tests."));

	varyingGroup->addChild(new BuiltinGlFrontFacingCase(testCtx, "gl_frontfacing", "gl_FrontFacing test"));

	return varyingGroup.release();
}

} // sr
} // vkt
